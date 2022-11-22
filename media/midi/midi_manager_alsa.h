// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
#define MEDIA_MIDI_MIDI_MANAGER_ALSA_H_

#include <alsa/asoundlib.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "device/udev_linux/scoped_udev.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"

namespace midi {

class MIDI_EXPORT MidiManagerAlsa final : public MidiManager {
 public:
  explicit MidiManagerAlsa(MidiService* service);

  MidiManagerAlsa(const MidiManagerAlsa&) = delete;
  MidiManagerAlsa& operator=(const MidiManagerAlsa&) = delete;

  ~MidiManagerAlsa() override;

  // MidiManager implementation.
  void StartInitialization() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp) override;

 private:
  friend class MidiManagerAlsaTest;
  FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ExtractManufacturer);
  FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ToMidiPortState);

  class AlsaCard;
  using AlsaCardMap = std::map<int, std::unique_ptr<AlsaCard>>;

  class MidiPort {
   public:
    enum class Type { kInput, kOutput };

    // The Id class is used to keep the multiple strings separate
    // but compare them all together for equality purposes.
    // The individual strings that make up the Id can theoretically contain
    // arbitrary characters, so unfortunately there is no simple way to
    // concatenate them into a single string.
    class Id final {
     public:
      Id();
      Id(const std::string& bus,
         const std::string& vendor_id,
         const std::string& model_id,
         const std::string& usb_interface_num,
         const std::string& serial);
      Id(const Id&);
      ~Id();
      bool operator==(const Id&) const;
      bool empty() const;

      std::string bus() const { return bus_; }
      std::string vendor_id() const { return vendor_id_; }
      std::string model_id() const { return model_id_; }
      std::string usb_interface_num() const { return usb_interface_num_; }
      std::string serial() const { return serial_; }

     private:
      std::string bus_;
      std::string vendor_id_;
      std::string model_id_;
      std::string usb_interface_num_;
      std::string serial_;
    };

    MidiPort(const std::string& path,
             const Id& id,
             int client_id,
             int port_id,
             int midi_device,
             const std::string& client_name,
             const std::string& port_name,
             const std::string& manufacturer,
             const std::string& version,
             Type type);

    MidiPort(const MidiPort&) = delete;
    MidiPort& operator=(const MidiPort&) = delete;

    ~MidiPort();

    // Gets a Value representation of this object, suitable for serialization.
    std::unique_ptr<base::Value::Dict> Value() const;

    // Gets a string version of Value in JSON format.
    std::string JSONValue() const;

    // Gets an opaque identifier for this object, suitable for using as the id
    // field in MidiPort.id on the web. Note that this string does not store
    // the full state.
    std::string OpaqueKey() const;

    // Checks for equality for connected ports.
    bool MatchConnected(const MidiPort& query) const;
    // Checks for equality for kernel cards with id, pass 1.
    bool MatchCardPass1(const MidiPort& query) const;
    // Checks for equality for kernel cards with id, pass 2.
    bool MatchCardPass2(const MidiPort& query) const;
    // Checks for equality for non-card clients, pass 1.
    bool MatchNoCardPass1(const MidiPort& query) const;
    // Checks for equality for non-card clients, pass 2.
    bool MatchNoCardPass2(const MidiPort& query) const;

    // accessors
    std::string path() const { return path_; }
    Id id() const { return id_; }
    std::string client_name() const { return client_name_; }
    std::string port_name() const { return port_name_; }
    std::string manufacturer() const { return manufacturer_; }
    std::string version() const { return version_; }
    int client_id() const { return client_id_; }
    int port_id() const { return port_id_; }
    int midi_device() const { return midi_device_; }
    Type type() const { return type_; }
    uint32_t web_port_index() const { return web_port_index_; }
    bool connected() const { return connected_; }

    // mutators
    void set_web_port_index(uint32_t web_port_index) {
      web_port_index_ = web_port_index;
    }
    void set_connected(bool connected) { connected_ = connected; }
    void Update(const std::string& path,
                int client_id,
                int port_id,
                const std::string& client_name,
                const std::string& port_name,
                const std::string& manufacturer,
                const std::string& version) {
      path_ = path;
      client_id_ = client_id;
      port_id_ = port_id;
      client_name_ = client_name;
      port_name_ = port_name;
      manufacturer_ = manufacturer;
      version_ = version;
    }

   private:
    // Immutable properties.
    const Id id_;
    const int midi_device_;

    const Type type_;

    // Mutable properties. These will get updated as ports move around or
    // drivers change.
    std::string path_;
    int client_id_;
    int port_id_;
    std::string client_name_;
    std::string port_name_;
    std::string manufacturer_;
    std::string version_;

    // Index for MidiManager.
    uint32_t web_port_index_ = 0;

    // Port is present in the ALSA system.
    bool connected_ = true;
  };

  class MidiPortStateBase {
   public:
    typedef std::vector<std::unique_ptr<MidiPort>>::iterator iterator;

    MidiPortStateBase(const MidiPortStateBase&) = delete;
    MidiPortStateBase& operator=(const MidiPortStateBase&) = delete;

    virtual ~MidiPortStateBase();

    // Given a port, finds a port in the internal store.
    iterator Find(const MidiPort& port);

    // Given a port, finds a connected port, using exact matching.
    iterator FindConnected(const MidiPort& port);

    // Given a port, finds a disconnected port, using heuristic matching.
    iterator FindDisconnected(const MidiPort& port);

    iterator begin() { return ports_.begin(); }
    iterator end() { return ports_.end(); }

   protected:
    MidiPortStateBase();
    iterator erase(iterator position) { return ports_.erase(position); }
    void push_back(std::unique_ptr<MidiPort> port) {
      ports_.push_back(std::move(port));
    }

   private:
    std::vector<std::unique_ptr<MidiPort>> ports_;
  };

  class TemporaryMidiPortState final : public MidiPortStateBase {
   public:
    iterator erase(iterator position) {
      return MidiPortStateBase::erase(position);
    }
    void push_back(std::unique_ptr<MidiPort> port) {
      MidiPortStateBase::push_back(std::move(port));
    }
  };

  class MidiPortState final : public MidiPortStateBase {
   public:
    MidiPortState();

    // Inserts a port at the end. Returns web_port_index.
    uint32_t push_back(std::unique_ptr<MidiPort> port);

   private:
    uint32_t num_input_ports_ = 0;
    uint32_t num_output_ports_ = 0;
  };

  class AlsaSeqState {
   public:
    enum class PortDirection { kInput, kOutput, kDuplex };

    AlsaSeqState();

    AlsaSeqState(const AlsaSeqState&) = delete;
    AlsaSeqState& operator=(const AlsaSeqState&) = delete;

    ~AlsaSeqState();

    void ClientStart(int client_id,
                     const std::string& client_name,
                     snd_seq_client_type_t type);
    bool ClientStarted(int client_id);
    void ClientExit(int client_id);
    void PortStart(int client_id,
                   int port_id,
                   const std::string& port_name,
                   PortDirection direction,
                   bool midi);
    void PortExit(int client_id, int port_id);
    snd_seq_client_type_t ClientType(int client_id) const;
    std::unique_ptr<TemporaryMidiPortState> ToMidiPortState(
        const AlsaCardMap& alsa_cards);

    int card_client_count() { return card_client_count_; }

   private:
    class Port {
     public:
      Port(const std::string& name, PortDirection direction, bool midi);

      Port(const Port&) = delete;
      Port& operator=(const Port&) = delete;

      ~Port();

      std::string name() const { return name_; }
      PortDirection direction() const { return direction_; }
      // True if this port is a MIDI port, instead of another kind of ALSA port.
      bool midi() const { return midi_; }

     private:
      const std::string name_;
      const PortDirection direction_;
      const bool midi_;
    };

    class Client {
     public:
      using PortMap = std::map<int, std::unique_ptr<Port>>;

      Client(const std::string& name, snd_seq_client_type_t type);

      Client(const Client&) = delete;
      Client& operator=(const Client&) = delete;

      ~Client();

      std::string name() const { return name_; }
      snd_seq_client_type_t type() const { return type_; }
      void AddPort(int addr, std::unique_ptr<Port> port);
      void RemovePort(int addr);
      PortMap::const_iterator begin() const;
      PortMap::const_iterator end() const;

     private:
      const std::string name_;
      const snd_seq_client_type_t type_;
      PortMap ports_;
    };

    std::map<int, std::unique_ptr<Client>> clients_;

    // This is the current number of clients we know about that have
    // cards. When this number matches alsa_card_midi_count_, we know
    // we are in sync between ALSA and udev. Until then, we cannot generate
    // MIDIConnectionEvents to web clients.
    int card_client_count_ = 0;
  };

  class AlsaCard {
   public:
    AlsaCard(udev_device* dev,
             const std::string& name,
             const std::string& longname,
             const std::string& driver,
             int midi_device_count);

    AlsaCard(const AlsaCard&) = delete;
    AlsaCard& operator=(const AlsaCard&) = delete;

    ~AlsaCard();
    std::string name() const { return name_; }
    std::string longname() const { return longname_; }
    std::string driver() const { return driver_; }
    std::string path() const { return path_; }
    std::string bus() const { return bus_; }
    std::string vendor_id() const { return vendor_id_; }
    std::string model_id() const { return model_id_; }
    std::string usb_interface_num() const { return usb_interface_num_; }
    std::string serial() const { return serial_; }
    int midi_device_count() const { return midi_device_count_; }
    std::string manufacturer() const { return manufacturer_; }

   private:
    FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ExtractManufacturer);

    // Extracts the manufacturer using heuristics and a variety of sources.
    static std::string ExtractManufacturerString(
        const std::string& udev_id_vendor,
        const std::string& udev_id_vendor_id,
        const std::string& udev_id_vendor_from_database,
        const std::string& name,
        const std::string& longname);

    const std::string name_;
    const std::string longname_;
    const std::string driver_;
    const std::string path_;
    const std::string bus_;
    const std::string vendor_id_;
    const std::string model_id_;
    const std::string usb_interface_num_;
    const std::string serial_;
    const int midi_device_count_;
    const std::string manufacturer_;
  };

  struct SndSeqDeleter {
    void operator()(snd_seq_t* seq) const { snd_seq_close(seq); }
  };

  struct SndMidiEventDeleter {
    void operator()(snd_midi_event_t* coder) const {
      snd_midi_event_free(coder);
    }
  };

  using SourceMap = std::unordered_map<int, uint32_t>;
  using OutPortMap = std::unordered_map<uint32_t, int>;
  using ScopedSndSeqPtr = std::unique_ptr<snd_seq_t, SndSeqDeleter>;
  using ScopedSndMidiEventPtr =
      std::unique_ptr<snd_midi_event_t, SndMidiEventDeleter>;

  // An internal callback that runs on MidiSendThread.
  void SendMidiData(MidiManagerClient* client,
                    uint32_t port_index,
                    const std::vector<uint8_t>& data);

  void EventLoop();
  void ProcessSingleEvent(snd_seq_event_t* event, base::TimeTicks timestamp);
  void ProcessClientStartEvent(int client_id);
  void ProcessPortStartEvent(const snd_seq_addr_t& addr);
  void ProcessClientExitEvent(const snd_seq_addr_t& addr);
  void ProcessPortExitEvent(const snd_seq_addr_t& addr);
  void ProcessUdevEvent(udev_device* dev);
  void AddCard(udev_device* dev);
  void RemoveCard(int number);

  // Updates port_state_ and Web MIDI state from alsa_seq_state_.
  void UpdatePortStateAndGenerateEvents();

  // Enumerates ports. Call once after subscribing to the announce port.
  void EnumerateAlsaPorts();
  // Enumerates udev cards. Call once after initializing the udev monitor.
  bool EnumerateUdevCards();
  // Returns true if successful.
  bool CreateAlsaOutputPort(uint32_t port_index, int client_id, int port_id);
  void DeleteAlsaOutputPort(uint32_t port_index);
  // Returns true if successful.
  bool Subscribe(uint32_t port_index, int client_id, int port_id);

  // Allocates new snd_midi_event_t instance and wraps it to return as
  // ScopedSndMidiEventPtr.
  ScopedSndMidiEventPtr CreateScopedSndMidiEventPtr(size_t size);

  // Members initialized in the constructor are below.
  // Our copies of the internal state of the ports of seq and udev.
  AlsaSeqState alsa_seq_state_;
  MidiPortState port_state_;

  // One input port, many output ports.
  base::Lock out_ports_lock_;
  OutPortMap out_ports_ GUARDED_BY(out_ports_lock_);

  // Mapping from ALSA client:port to our index.
  SourceMap source_map_;

  // Mapping from card to devices.
  AlsaCardMap alsa_cards_;

  // This is the current count of midi devices across all cards we know
  // about. When this number matches card_client_count_ in AlsaSeqState,
  // we are safe to generate MIDIConnectionEvents. Otherwise we need to
  // wait for our information from ALSA and udev to get back in sync.
  int alsa_card_midi_count_ = 0;

  // ALSA seq handles and ids.
  ScopedSndSeqPtr in_client_;
  int in_client_id_;
  ScopedSndSeqPtr out_client_ GUARDED_BY(out_client_lock_);
  base::Lock out_client_lock_;
  int out_client_id_;
  int in_port_id_;

  // ALSA event -> MIDI coder.
  ScopedSndMidiEventPtr decoder_;

  // udev, for querying hardware devices.
  device::ScopedUdevPtr udev_;
  device::ScopedUdevMonitorPtr udev_monitor_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
