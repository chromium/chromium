// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/task_viewer/task_viewer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/catalog/public/mojom/catalog.mojom.h"
#include "services/catalog/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "ui/base/models/table_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace task_viewer {
namespace {

using service_manager::mojom::RunningServiceInfoPtr;

class TaskViewerContents
    : public views::WidgetDelegateView,
      public ui::TableModel,
      public views::ButtonListener,
      public service_manager::mojom::ServiceManagerListener {
 public:
  TaskViewerContents(
      TaskViewer* task_viewer,
      service_manager::mojom::ServiceManagerListenerRequest request,
      catalog::mojom::CatalogPtr catalog)
      : task_viewer_(task_viewer),
        binding_(this, std::move(request)),
        catalog_(std::move(catalog)),
        table_view_(nullptr),
        table_view_parent_(nullptr),
        kill_button_(
            views::MdTextButton::Create(this,
                                        base::ASCIIToUTF16("Kill Process"))),
        observer_(nullptr),
        weak_ptr_factory_(this) {
    // We don't want to show an empty UI on startup, so just block until we
    // receive the initial set of applications.
    binding_.WaitForIncomingMethodCall();

    table_view_ = new views::TableView(this, GetColumns(), views::TEXT_ONLY,
                                       false);
    SetBackground(views::CreateStandardPanelBackground());

    table_view_parent_ = table_view_->CreateParentIfNecessary();
    AddChildView(table_view_parent_);

    AddChildView(kill_button_);
  }
  ~TaskViewerContents() override {
    table_view_->SetModel(nullptr);
    task_viewer_->RemoveWindow(GetWidget());
  }

 private:
  struct InstanceInfo {
    InstanceInfo(const service_manager::Identity& identity, base::ProcessId pid)
        : identity(identity), pid(pid) {}
    service_manager::Identity identity;
    uint32_t pid;
    std::string display_name;
  };


  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Tasks");
  }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  gfx::ImageSkia GetWindowAppIcon() override {
    // TODO(jamescook): Create a new .pak file for this app and make a custom
    // icon, perhaps one that looks like the Chrome OS task viewer icon.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    return *rb.GetImageSkiaNamed(IDR_NOTIFICATION_SETTINGS);
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(10, 10);

    gfx::Size ps = kill_button_->GetPreferredSize();
    bounds.set_height(bounds.height() - ps.height() - 10);

    kill_button_->SetBounds(bounds.width() - ps.width(),
                            bounds.bottom() + 10,
                            ps.width(), ps.height());
    table_view_parent_->SetBoundsRect(bounds);
  }

  // Overridden from ui::TableModel:
  int RowCount() override {
    return static_cast<int>(instances_.size());
  }
  base::string16 GetText(int row, int column_id) override {
    switch (column_id) {
      case 0:
        DCHECK(row < static_cast<int>(instances_.size()));
        return base::UTF8ToUTF16(instances_[row]->display_name);
      case 1:
        DCHECK(row < static_cast<int>(instances_.size()));
        return base::UTF8ToUTF16(instances_[row]->identity.name());
      case 2:
        DCHECK(row < static_cast<int>(instances_.size()));
        return base::IntToString16(instances_[row]->pid);
      default:
        NOTREACHED();
        break;
    }
    return base::string16();
  }
  void SetObserver(ui::TableModelObserver* observer) override {
    observer_ = observer;
  }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(sender, kill_button_);
    DCHECK_EQ(table_view_->selection_model().size(), 1UL);
    int row = table_view_->FirstSelectedRow();
    DCHECK(row < static_cast<int>(instances_.size()));
    base::Process process = base::Process::Open(instances_[row]->pid);
    process.Terminate(9, true);
  }

  // Overridden from service_manager::mojom::ServiceManagerListener:
  void OnInit(std::vector<RunningServiceInfoPtr> instances) override {
    // This callback should only be called with an empty model.
    DCHECK(instances_.empty());
    std::vector<std::string> names;
    names.reserve(instances.size());
    for (size_t i = 0; i < instances.size(); ++i) {
      const service_manager::Identity& identity = instances[i]->identity;
      InsertInstance(identity, instances[i]->pid);
      names.push_back(identity.name());
    }
    catalog_->GetEntries(std::move(names),
                         base::Bind(&TaskViewerContents::OnGotCatalogEntries,
                                    weak_ptr_factory_.GetWeakPtr()));
  }
  void OnServiceCreated(RunningServiceInfoPtr instance) override {
    service_manager::Identity identity = instance->identity;
    DCHECK(!ContainsIdentity(identity));
    InsertInstance(identity, instance->pid);
    observer_->OnItemsAdded(static_cast<int>(instances_.size()), 1);
    std::vector<std::string> names;
    names.push_back(identity.name());
    catalog_->GetEntries(std::move(names),
                         base::Bind(&TaskViewerContents::OnGotCatalogEntries,
                                    weak_ptr_factory_.GetWeakPtr()));
  }
  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      if ((*it)->identity == identity) {
        (*it)->pid = pid;
        observer_->OnItemsChanged(
          static_cast<int>(it - instances_.begin()), 1);
        return;
      }
    }
  }
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {
  }
  void OnServiceStopped(const service_manager::Identity& identity) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      if ((*it)->identity == identity) {
        observer_->OnItemsRemoved(
            static_cast<int>(it - instances_.begin()), 1);
        instances_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }
  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {}

  bool ContainsIdentity(const service_manager::Identity& identity) const {
    for (auto& it : instances_) {
      if (it->identity == identity)
        return true;
    }
    return false;
  }

  void InsertInstance(const service_manager::Identity& identity, uint32_t pid) {
    instances_.push_back(std::make_unique<InstanceInfo>(identity, pid));
  }

  void OnGotCatalogEntries(std::vector<catalog::mojom::EntryPtr> entries) {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      for (auto& entry : entries) {
        if (entry->name == (*it)->identity.name()) {
          (*it)->display_name = entry->display_name;
          observer_->OnItemsChanged(
              static_cast<int>(it - instances_.begin()), 1);
          break;
        }
      }
    }
  }

  static std::vector<ui::TableColumn> GetColumns() {
    std::vector<ui::TableColumn> columns;

    ui::TableColumn name_column;
    name_column.id = 0;
    // TODO(beng): use resources.
    name_column.title = base::ASCIIToUTF16("Name");
    name_column.width = -1;
    name_column.percent = 0.4f;
    name_column.sortable = true;
    columns.push_back(name_column);

    ui::TableColumn url_column;
    url_column.id = 1;
    // TODO(beng): use resources.
    url_column.title = base::ASCIIToUTF16("URL");
    url_column.width = -1;
    url_column.percent = 0.4f;
    url_column.sortable = true;
    columns.push_back(url_column);

    ui::TableColumn pid_column;
    pid_column.id = 2;
    // TODO(beng): use resources.
    pid_column.title  = base::ASCIIToUTF16("PID");
    pid_column.width = 50;
    pid_column.sortable = true;
    columns.push_back(pid_column);

    return columns;
  }

  TaskViewer* task_viewer_;
  mojo::Binding<service_manager::mojom::ServiceManagerListener> binding_;
  catalog::mojom::CatalogPtr catalog_;

  views::TableView* table_view_;
  views::View* table_view_parent_;
  views::MdTextButton* kill_button_;
  ui::TableModelObserver* observer_;

  std::vector<std::unique_ptr<InstanceInfo>> instances_;

  base::WeakPtrFactory<TaskViewerContents> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskViewerContents);
};

}  // namespace

TaskViewer::TaskViewer() {
  registry_.AddInterface<::mash::mojom::Launchable>(
      base::Bind(&TaskViewer::Create, base::Unretained(this)));
}
TaskViewer::~TaskViewer() = default;

void TaskViewer::RemoveWindow(views::Widget* widget) {
  auto it = std::find(windows_.begin(), windows_.end(), widget);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty())
    context()->QuitNow();
}

void TaskViewer::OnStart() {
  views::AuraInit::InitParams params;
  params.connector = context()->connector();
  params.identity = context()->identity();
  aura_init_ = views::AuraInit::Create(params);
  if (!aura_init_)
    context()->QuitNow();
}

void TaskViewer::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void TaskViewer::Launch(uint32_t what, mojom::LaunchMode how) {
  bool reuse = how == mojom::LaunchMode::REUSE ||
               how == mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }

  service_manager::mojom::ServiceManagerPtr service_manager;
  context()->connector()->BindInterface(service_manager::mojom::kServiceName,
                                        &service_manager);

  catalog::mojom::CatalogPtr catalog;
  context()->connector()->BindInterface(catalog::mojom::kServiceName, &catalog);

  service_manager::mojom::ServiceManagerListenerPtr listener;
  TaskViewerContents* task_viewer = new TaskViewerContents(
      this, mojo::MakeRequest(&listener), std::move(catalog));
  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      task_viewer, nullptr, gfx::Rect(10, 10, 500, 500));
  window->Show();
  windows_.push_back(window);
  service_manager->AddListener(std::move(listener));
}

void TaskViewer::Create(::mash::mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace task_viewer
}  // namespace mash
