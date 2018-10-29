// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/catalog_viewer/catalog_viewer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/catalog/public/mojom/catalog.mojom.h"
#include "services/catalog/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "ui/base/models/table_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace catalog_viewer {
namespace {

class CatalogViewerContents : public views::WidgetDelegateView,
                              public ui::TableModel,
                              public views::TextfieldController {
 public:
  CatalogViewerContents(CatalogViewer* catalog_viewer,
                        catalog::mojom::CatalogPtr catalog)
      : catalog_viewer_(catalog_viewer),
        catalog_(std::move(catalog)),
        table_view_(nullptr),
        table_view_parent_(nullptr),
        observer_(nullptr),
        capability_(new views::Textfield) {
    constexpr int kPadding = 5;
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kPadding)));
    SetBackground(views::CreateStandardPanelBackground());

    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>(this));

    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                       views::GridLayout::USE_PREF, 0, 0);
    columns->AddPaddingColumn(0, kPadding);
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                       views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    layout->AddView(new views::Label(base::WideToUTF16(L"Capability:")));
    layout->AddView(capability_);
    capability_->set_controller(this);

    layout->StartRowWithPadding(1, 0, 0, kPadding);
    table_view_ =
        new views::TableView(this, GetColumns(), views::TEXT_ONLY, false);
    table_view_parent_ = table_view_->CreateParentIfNecessary();
    layout->AddView(table_view_parent_, 3, 1, views::GridLayout::FILL,
                    views::GridLayout::FILL);

    GetAllEntries();
  }
  ~CatalogViewerContents() override {
    table_view_->SetModel(nullptr);
    catalog_viewer_->RemoveWindow(GetWidget());
  }

 private:
  struct Entry {
    Entry(const std::string& name, const std::string& url)
        : name(name), url(url) {}
    std::string name;
    std::string url;
  };


  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Applications");
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

  // Overridden from ui::TableModel:
  int RowCount() override {
    return static_cast<int>(entries_.size());
  }
  base::string16 GetText(int row, int column_id) override {
    switch (column_id) {
      case 0:
        DCHECK(row < static_cast<int>(entries_.size()));
        return base::UTF8ToUTF16(entries_[row].name);
      case 1:
        DCHECK(row < static_cast<int>(entries_.size()));
        return base::UTF8ToUTF16(entries_[row].url);
      default:
        NOTREACHED();
        break;
    }
    return base::string16();
  }

  // Overriden from views::TextFieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::ET_KEY_PRESSED ||
        key_event.key_code() != ui::VKEY_RETURN)
      return false;

    if (sender->text().length()) {
      catalog_->GetEntriesProvidingCapability(
          base::UTF16ToUTF8(sender->text()),
          base::Bind(&CatalogViewerContents::OnReceivedEntries,
                     base::Unretained(this)));
    } else {
      GetAllEntries();
    }

    return true;
  }

  void GetAllEntries() {
    // We don't want to show an empty UI so we just block until we have all the
    // data. GetEntries is a sync call.
    std::vector<catalog::mojom::EntryPtr> entries;
    if (catalog_->GetEntries(base::nullopt, &entries))
      UpdateEntries(entries);
  }

  void OnReceivedEntries(std::vector<catalog::mojom::EntryPtr> entries) {
    UpdateEntries(entries);
  }

  void UpdateEntries(const std::vector<catalog::mojom::EntryPtr>& entries) {
    entries_.clear();
    for (auto& entry : entries)
      entries_.push_back(Entry(entry->display_name, entry->name));
    observer_->OnModelChanged();
  }

  void SetObserver(ui::TableModelObserver* observer) override {
    observer_ = observer;
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

    return columns;
  }

  CatalogViewer* catalog_viewer_;
  catalog::mojom::CatalogPtr catalog_;

  views::TableView* table_view_;
  views::View* table_view_parent_;
  ui::TableModelObserver* observer_;
  views::Textfield* capability_;

  std::vector<Entry> entries_;

  DISALLOW_COPY_AND_ASSIGN(CatalogViewerContents);
};

}  // namespace

CatalogViewer::CatalogViewer() {
  registry_.AddInterface<mojom::Launchable>(
      base::Bind(&CatalogViewer::Create, base::Unretained(this)));
}
CatalogViewer::~CatalogViewer() = default;

void CatalogViewer::RemoveWindow(views::Widget* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty())
    context()->QuitNow();
}

void CatalogViewer::OnStart() {
  views::AuraInit::InitParams params;
  params.connector = context()->connector();
  params.identity = context()->identity();
  aura_init_ = views::AuraInit::Create(params);
  if (!aura_init_)
    context()->QuitNow();
}

void CatalogViewer::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void CatalogViewer::Launch(uint32_t what, mojom::LaunchMode how) {
  bool reuse = how == mojom::LaunchMode::REUSE ||
               how == mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }
  catalog::mojom::CatalogPtr catalog;
  context()->connector()->BindInterface(catalog::mojom::kServiceName, &catalog);

  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      new CatalogViewerContents(this, std::move(catalog)), nullptr,
      gfx::Rect(25, 25, 500, 600));
  window->Show();
  windows_.push_back(window);
}

void CatalogViewer::Create(mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace catalog_viewer
}  // namespace mash
